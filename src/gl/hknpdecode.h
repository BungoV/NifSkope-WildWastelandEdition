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
	/*! convex only: Havok's per-face minHalfAngle byte, parallel to faces.
	 *
	 * The 4th byte of each face entry. It is NOT a flags constant -- across the 76
	 * vanilla polytopes it takes dozens of values with no clustering, which is what
	 * a quantized angle looks like. Capsules are the exception, where every face
	 * carries 4. An encoder has to preserve or derive it, so it is kept.
	 */
	QVector<quint8> faceAngles;
	//! convex only: face planes (nx, ny, nz, d)
	QVector<Vector4> planes;
	float convexRadius = 0.0f;
	/*! the u32 at shape+0x10, one word before the radius.
	 *
	 * Looks like a type tag plus flags: `010001c3` on capsules, `01000111` on
	 * spheres, `01000143` on 74 of 76 polytopes and `01000043` on the other 2.
	 * Kept so a re-encode can reproduce it rather than assume the common value.
	 */
	quint32 shapeFlags = 0;

	/*! hknpShapeMassProperties, reached via hkRefCountedProperties at shape+0x20.
	 *
	 * Only convex polytopes carry one; capsules and spheres have none at all (the
	 * brahmin ragdoll, 39 capsules and spheres, has zero of these objects).
	 *
	 * The 0x30-byte object is 16 bytes of header then hkCompressedMassProperties:
	 * a packed centre of mass at +0x10, a packed inertia diagonal at +0x18, an
	 * 8-byte major-axis frame at +0x20, then float volume at +0x28 and float mass
	 * at +0x2c.
	 *
	 * The +0x20 field was once noted here as "one constant value in every file
	 * seen". It is not: over 76 vanilla objects it takes 23 distinct values. The
	 * earlier sample was simply too small, the same way the capsule's core box
	 * looked like an AABB until tilted capsules turned up. Its packing is NOT
	 * decoded -- as four int16 over 32768 the norm lands near sqrt(3) rather than
	 * 1, and the largest component does not reliably saturate -- so it is carried
	 * as opaque bytes in massMajorAxis rather than guessed at.
	 *
	 * All of it is computed on the shape with its FACE PLANES PUSHED OUT by the
	 * convex radius -- not on the rounded Minkowski sum of hull and sphere. For a
	 * box that is exactly the box grown by 2r on each edge, and on the workshop
	 * turret's 8 polytopes that reproduces the stored volume to 0.000% where the
	 * Steiner formula for a properly rounded box is out by up to 1.1%. So Havok
	 * treats the radius as an offset of the support planes, which is also how it
	 * collides them.
	 */
	bool hasMassProps = false;
	Vector3 massCom;            //!< centre of mass, shape space
	/*! Inertia diagonal as stored, which is 1.5x the physical inertia.
	 *
	 * Measured exactly -- 8 shapes, 24 components, ratio 1.5000 throughout,
	 * against the axis-aligned inertia of the expanded box at the stored mass. Why
	 * Havok scales it is not established; that it does is not in doubt, and an
	 * encoder only needs the factor. massInertia() returns the physical value.
	 */
	Vector3 massInertiaRaw;
	float massVolume = 0.0f;
	float massMass = 0.0f;      //!< equals the volume in every vanilla case (density 1)
	//! the 8 bytes at +0x20 verbatim: the major-axis frame, packing undecoded
	quint64 massMajorAxis = 0;
	//! byte offset of the hknpShapeMassProperties object, or -1
	qsizetype massPropsOffset = -1;
	/*! That object as stored, for the same reason as rawData.
	 *
	 * A packed vector whose three mantissas are all zero keeps whatever exponent
	 * Havok's arithmetic landed on, and zero mantissas record no magnitude, so the
	 * original is not recoverable -- 24 of 269 corpus objects carry one. Cleared
	 * by a caller that edited the shape, which then gets the derived value.
	 */
	QByteArray massRawData;
	//! the physical inertia diagonal, undoing Havok's 1.5 scale
	Vector3 massInertia() const { return massInertiaRaw / 1.5f; }
	bool isConvex = false;
	/*! Havok material CRC (u32 at shape+0x18; same value as the legacy
	 * HavokMaterial enum). 0 = unknown.
	 *
	 * This is the EFFECTIVE material: for a convex shape owned by a body that
	 * names one, the body's material is resolved over the shape's own. That is
	 * what a user should see, but it is not what the object holds -- see
	 * shapeMaterialCRC.
	 */
	quint32 materialCRC = 0;
	/*! The material actually stored at shape+0x18, never overwritten.
	 *
	 * An encoder has to write this one. Using the resolved value instead is
	 * invisible on actor skeletons, where the two agree, and wrong on static
	 * architecture, where they do not.
	 */
	quint32 shapeMaterialCRC = 0;

	//! primitive interpretation: 0 generic, 1 sphere, 2 capsule
	int primType = 0;
	Vector3 capA, capB;            //!< capsule end points / sphere center (capA)
	/*! Sphere / capsule radius: the OUTER radius, the one that collides.
	 *
	 * A capsule stores a small core box plus convexRadius, and the solid is that
	 * box with its support planes pushed out by the radius -- so it is a rounded
	 * box, not exactly a capsule, and one radius cannot describe it. The
	 * half-width runs from convexRadius + padding facing a face to
	 * convexRadius + padding * sqrt(2) facing a corner, 0.42% apart. This takes
	 * the circumscribed value, which never under-states the solid.
	 *
	 * With padding at convexRadius/99 that is convexRadius * 1.014288. Reading it
	 * as convexRadius * 1.0175 -- which is what the old closest-point-on-SEGMENT
	 * measurement gave, the clamp folding in the axial overshoot -- inflates every
	 * capsule by 0.35%.
	 */
	float primRadius = 0.0f;
	/*! Capsules only: the 8 core-box corners exactly as stored, in index order.
	 *
	 * Kept because the box carries a roll about the capsule axis that nothing
	 * else records. A capsule is rotationally symmetric, so the roll is
	 * geometrically inert, but it is needed to rewrite the shape byte for byte
	 * and cannot be recovered from (capA, capB, radius) -- see
	 * HknpCapsuleInput::frameU.
	 */
	QVector<Vector3> coreVerts;
	//! how far the core box is grown past the segment on each local axis, which
	//! is what makes the solid fatter than convexRadius. Near convexRadius/99,
	//! but measured per shape -- see HknpCapsuleInput::padding.
	float corePadding = 0.0f;

	//! byte offset of this shape object within the packfile blob -- absolute, the
	//! virtual fixups are resolved against __data__ at decode time. Lets a
	//! re-encode be diffed against the bytes it came from. -1 = unknown.
	qsizetype rawOffset = -1;
	/*! The shape object exactly as stored, so an UNTOUCHED shape can be written
	 * back unchanged instead of re-derived.
	 *
	 * A capsule's core box is not stored, it is computed from the segment, the
	 * radius and the roll, and recomputing it lands a few ULP away -- around 750
	 * differing bytes on an 18-capsule ragdoll, none of them meaning anything
	 * (worst vertex error 1e-06 m). Editing one joint limit should not perturb
	 * every capsule in the file, so a writer starts from these bytes.
	 *
	 * A caller that HAS edited the shape clears this, and the encoder derives.
	 * Same contract as HknpConstraint::rawData.
	 */
	QByteArray rawData;

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
	//! cinfo +0x3c: the position's w lane. Zero on 813 of the 857 corpus bodies
	//! and one of 38 values on the rest, so it is residue rather than a field --
	//! carried because a rewrite cannot invent it. See HknpConstraint::rawData.
	quint32 positionW = 0;

	/*! cinfo +0x0c: which dyn_motion / dyn_inertia entry this body uses.
	 *
	 * NOT the body's own index -- the arrays are shorter than the body list on
	 * any ragdoll with static parts, and limbs share motions. 0x7fffffff means
	 * static, which is what hasMotion reports. -1 here means static too.
	 */
	int motionIndex = -1;

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
	/*! dyn_inertia +0x30: a world-space position, and NOT the body's own.
	 *
	 * Distinct from cinfo's position on 848 of 857 bodies, by 0.67 m at the
	 * median. Its magnitude tracks the skeleton's scale (median 0.99, up to 12.9
	 * on Liberty Prime), so it is a point in the ragdoll's space rather than a
	 * local offset. The natural reading is the motion's centre of mass, which is
	 * where hknpMotion keeps one and would sit mid-limb as this does -- but that
	 * is the reading, not a measurement, so the name says what it is.
	 */
	Vector3 motionCom;
	quint32 motionComW = 0;             //!< dyn_inertia +0x3c, the w lane beside it
	/*! dyn_inertia +0x40: a unit quaternion, xyzw. Unit to 1e-5 on all 857.
	 *
	 * Also not a duplicate of the body's orientation -- it differs bitwise on all
	 * 857 -- so the motion carries its own frame.
	 */
	Quat motionOrientation = Quat( 1, 0, 0, 0 );
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
	//! the wrapper's break threshold (+0x20). Only 3 exist in the corpus, two at
	//! 0 and one at 0.01, so it is decoded rather than defaulted -- writing the
	//! wrong one silently changes when a joint snaps.
	float breakThreshold = 0.0f;

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

	//! byte offset of the constraint-data object in the packfile blob, or -1
	qsizetype rawOffset = -1;
	/*! The constraint-data object exactly as stored.
	 *
	 * Constraint objects are fixed-size templates -- 416 bytes for a ragdoll, 304
	 * for a limited hinge, one size each across the whole corpus -- and most of
	 * their content is neither modelled above nor constant. Some of it is not even
	 * meaningful: one vanilla pivot's w slot holds 0x98d3b2b5, uninitialised
	 * padding Havok never cleared. Rewriting from the original bytes and
	 * overwriting only the modelled fields is therefore the only way an edit to a
	 * limit cannot disturb anything else.
	 */
	QByteArray rawData;
	/*! Offsets inside the constraint object that hold an hkpPositionConstraintMotor
	 * pointer, so a rewrite can re-bind them.
	 *
	 * A ragdoll constraint has THREE, at +0x100/+0x108/+0x110 -- the twist, plane
	 * and cone motors of its RAGDOLL_MOTOR atom -- and every one of the 520 points
	 * at the single motor the whole file shares. A limited hinge has one at +0xd0
	 * on 244 of 246. The three constraints with none are why this is recorded
	 * rather than assumed from the kind.
	 */
	QVector<qsizetype> motorPointers;
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
	/*! Reference-pose scale. NOT (1,1,1): 767 of the 804 corpus bones carry
	 * 0.99999994, one ULP below 1, and only the 37 roots carry exactly 1.
	 *
	 * A probe printing this as %.4f showed "1.0000" and I recorded it as unity.
	 * Formatted output has now hidden four separate fields in this format --
	 * see also HknpCompound::Instance::wPayload.
	 */
	Vector3 scale = Vector3( 1, 1, 1 );
	//! w lanes of the pose's translation and scale, which are not derivable
	quint32 poseTransW = 0, poseScaleW = 0;
};

/*! One hknpDynamicCompoundShape / hknpStaticCompoundShape as an OBJECT.
 *
 * Layout measured over 14 corpus compounds with no exceptions: the object is
 * `0xD0 + count * 0x80` bytes, the instance hkArray sits at +0x60 (pointer patched
 * by a LOCAL fixup to +0xD0, count at +0x68, count|0x80000000 at +0x6c), a GLOBAL
 * fixup at +0xC0 reaches the matching CompoundShapeData, and each 0x80-byte
 * instance holds three rotation rows, a translation at +0x30, a scale at +0x40 and
 * a child pointer at +0x50 patched by another GLOBAL fixup.
 *
 * Every one of those pointer slots holds RAW ZERO in the file -- checked on all 60
 * instances. The binding lives entirely in the fixup tables, which is why a
 * compound cannot be written as a self-contained blob the way a capsule can.
 */
struct HknpCompound
{
	//! byte offset of the compound object within the packfile blob, or -1
	qsizetype rawOffset = -1;
	bool dynamic = true;
	quint32 shapeFlags = 0;     //!< +0x10; 02020004 / 02030004 / 02040004 seen
	quint32 materialCRC = 0;    //!< +0x18

	Vector3 aabbMin;            //!< +0x80
	Vector3 aabbMax;            //!< +0x90
	/*! +0x70 .. +0xCF verbatim, the AABB included.
	 *
	 * Most of this region is NOT understood. A first pass scanned only the header
	 * up to +0x70 and concluded everything past the instance-array descriptor was
	 * zero; it is not, and an encoder built on that would have written a compound
	 * with no bounds. Carried whole so a rewrite reproduces it instead of
	 * inventing it -- the same treatment as the mass properties' major-axis frame.
	 */
	QByteArray headerTail;
	//! the body whose cinfo names this compound, or -1
	int bodyId = -1;
	/*! Indices into HknpSystem::shapes, parallel to instances.
	 *
	 * Decoding flattens a compound into one shape per instance, which is what
	 * drawing and simulation want but loses which shapes belonged to which
	 * compound. A writer needs it back, and it cannot be recovered from bodyId
	 * alone: every child of a compound carries the SAME body.
	 */
	QVector<int> children;
	/*! The matching hknpCompoundShapeData object, class name and bytes.
	 *
	 * Not decoded -- 224 bytes for 2 instances, 288 for 3, 352 for 4, with only a
	 * dozen non-zero words and no reading established for any of them. Carried
	 * whole, the same treatment as the major-axis frame. It is emitted AFTER the
	 * children even though its pointer at +0xC0 sits below theirs.
	 */
	QString dataClassName;
	QByteArray dataRawData;
	//! LOCAL fixups inside that object, relative to its start. All 6 in the
	//! corpus carry exactly one, +0x10 -> +0x40. Recorded rather than assumed:
	//! carrying the bytes but dropping the fixup writes a null pointer.
	QVector<QPair<qsizetype, qsizetype>> dataLocal;

	struct Instance
	{
		Vector3 rotRows[3] = { Vector3( 1, 0, 0 ), Vector3( 0, 1, 0 ), Vector3( 0, 0, 1 ) };
		Vector3 trans;
		Vector3 scale = Vector3( 1, 1, 1 );
		/*! The w slots of all three rotation rows and of the translation.
		 *
		 * None of them is padding. Rows 0 and 2 and the translation hold 0.5 with a
		 * payload in the low mantissa bits, the way a hull vertex carries its index
		 * -- observed values run 64/66/70, multiples of 16 up to 1440, and small
		 * integers, so at least one looks like a byte offset and one like an index.
		 * Row 1 holds a zero whose SIGN varies: three corpus instances carry
		 * negative zero.
		 *
		 * Both were missed the first time round for the same reason. Printing the
		 * payload slots as %.3f shows "0.500" and hides the mantissa entirely, and
		 * negative zero prints as "0.000" and compares equal to 0.0f. Only a byte
		 * comparison sees either. All four are carried verbatim.
		 *
		 * The scale w is not here: it is 0x3f800000 on all 60 corpus instances,
		 * checked as a bit pattern rather than as a printed float.
		 */
		quint32 wPayload[4] = { 0x3f000000u, 0u, 0x3f000000u, 0x3f000000u };
	};
	QVector<Instance> instances;
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
	//! byte offset of the hkaSkeleton object in the packfile blob, or -1
	qsizetype skeletonRawOffset = -1;
	//! byte offset of the hknpRagdollData root object, or -1
	qsizetype ragdollRawOffset = -1;
	/*! hknpRagdollData +0x60: the ragdoll's shape list, as BODY indices.
	 *
	 * It holds the same set of shape pointers as the body cinfos -- a bijection
	 * on all 37 corpus ragdolls -- but in a different order on 36 of them, so it
	 * is a permutation of the body list and not a copy of it.
	 *
	 * The order is the NIF's own: the sequence the bhkNPCollisionObject blocks
	 * appear in, which matches on 35 of 37 with the other two being parts kits
	 * whose spare bodies have no node at all. It is therefore NOT derivable from
	 * the packfile -- a writer inside a NIF has it, a standalone rewrite does not
	 * -- which is why the decode records it rather than reconstructing it.
	 *
	 * A depth-first walk of the bone tree reproduces it on 32 of 34, which is
	 * exactly the kind of near-miss that has been wrong every previous time this
	 * session. Node order is right for all of them.
	 */
	QVector<int> shapeListOrder;
	bool positionalBodies = false;
	/*! The compound shapes as OBJECTS, alongside the flattened child shapes.
	 *
	 * Decoding flattens a compound into one HknpShape per instance, each carrying
	 * the instance transform, which is what drawing and simulation want. Nothing
	 * then represents the compound itself, so a writer has nothing to reproduce --
	 * hence this parallel list.
	 */
	QVector<HknpCompound> compounds;
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
